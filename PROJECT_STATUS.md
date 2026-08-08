# Trace2D Project Status

Last repository-state update: **2026-08-08**

This document is the operational handoff for the next contributor or coding agent. Live repository state wins over stale prose.

## Current phase

**Public Alpha released — fixed post-alpha P6 execution is active.**

`v0.1.0-alpha.1` was published on 2026-08-08 and the repository is Public under the MIT License. The first release proves one complete minimal agent-first 2D development loop:

```text
text-authored scene
  -> build
  -> deterministic headless run
  -> explicit frame step
  -> semantic state query
  -> virtual input
  -> gameplay assertion
  -> ordered 2D render
  -> frame-specific visual capture
```

Public Alpha completion is recorded in Issue #14. Post-alpha work must extend this proven contract rather than replace it.

## Next execution order — owner-fixed

The repository owner fixed this implementation order on **2026-08-08**.

**Future coding agents must follow this sequence unless the repository owner explicitly changes it. Do not reorder, skip, parallelize, or substitute later roadmap work because another task appears more attractive.**

1. **#40 — deterministic texture asset cache/import slice**
2. **#42 — text rendering and basic UI primitives**
3. **#43 — semantic UI tree and agent interaction**
4. **#39 — MCP transport over the completed protocol-independent agent/UI facade**
5. **#41 — reproducible renderer performance workloads**
6. return to umbrella **#13** and split the next practical breadth item into a narrow issue before implementation: physics/Box2D, sprite animation, or safe hot reload

### Execution rule

- Work only on the first incomplete and unblocked item above.
- If that item has an active PR, finish/repair that PR before starting anything else.
- Merge with green CI, update this file, then advance exactly one step.
- If an earlier task reveals a genuine prerequisite, implement only the smallest prerequisite necessary and keep the owner-fixed sequence intact.
- Do **not** start #39 MCP before #43 semantic UI is complete.
- MCP is transport, not the engine API. It must expose an already-complete protocol-independent agent/UI vocabulary rather than drive engine architecture.
- UI automation is semantic-first: stable identity/role/name plus structured state/actions. Coordinates may be observable bounds, but must not be the primary automation identity when semantic identity exists.
- Structured state beats pixel inference for gameplay and UI assertions.

**Active next implementation task: #40.**

## Why this order

The asset slice comes first because practical authored UI/text needs deterministic project-relative resource identity and reuse. Text rendering/basic UI then establishes the smallest engine-owned UI state and layout foundation. Semantic UI automation builds on that foundation before MCP so the transport layer exposes a stable final vocabulary instead of forcing a second redesign. Renderer benchmarking follows after the agent-facing breadth is in place so later optimization remains measurement-driven.

## P6 umbrella

Issue #13 is the parent roadmap for the practical authored-game slice. The fixed child sequence is:

```text
#40 assets
  -> #42 text/basic UI
  -> #43 semantic UI automation
  -> #39 MCP transport
  -> #41 renderer workloads
  -> split next #13 breadth item
```

Do not treat #13 as permission to start Box2D, animation, hot reload, editor work, or other breadth before the fixed child sequence completes.

## Semantic UI target

The semantic UI milestone (#43) is not complete until supported controls expose structured state such as:

- stable semantic identity
- role
- name
- bounds
- visible
- enabled
- focused
- text/value where applicable

The protocol-independent Agent facade must support semantic operations conceptually equivalent to:

```text
query role=button name="Start Game"
inspect enabled/visible/bounds
activate semantic control
query role=textbox name="Player Name"
focus control
input text
assert resulting UI/game state
```

Headless semantic UI tests must not require renderer initialization. MCP support comes only afterward in #39.

## Public Alpha release record

- [x] P0-P5 technical milestones complete
- [x] Public Alpha vertical sample — PR #32
- [x] measured contiguous same-texture GPU instancing — PR #34
- [x] repository quality gates — PR #35 / CI #100
- [x] MIT license-required release gate — PR #36 / CI #103
- [x] release-ready documentation — PR #37 / CI #105
- [x] post-public documentation cleanup — PR #38 / CI #107
- [x] repository visibility Public
- [x] release/tag `v0.1.0-alpha.1`
- [x] root MIT `LICENSE`
- [x] Issue #14 closed completed

## Validation policy

Release-facing CI remains the baseline for changes that can affect the supported developer path:

- `release-audit` using `./scripts/release_audit.ps1 -RequireLicense`
- `windows-msvc` configure/build/full CTest
- `clean-clone-quick-start` using the README-pinned vcpkg baseline

New machine-facing capabilities require deterministic automated tests when practical. A semantic UI or MCP feature without headless coverage is incomplete unless its PR documents a concrete reason.

GPU presentation itself is not a hosted-runner requirement. Backend-independent state, layout, query, input, assertion, camera, visibility, batching measurement, capture-layout, and artifact contracts should remain CI-testable without an interactive GPU/window.

## Architecture invariants

- `engine/core` has no SDL dependency.
- SDL-specific ownership/types stay behind platform/render boundaries.
- renderer GPU state is presentation state and never authoritative simulation or UI state.
- runtime/scene/input/agent/testing and future engine-owned UI state do not depend on MCP transport.
- MCP/JSON-RPC/CLI are adapters over protocol-independent engine/agent contracts.
- persistent renderer resources are setup or capacity/size-dependent state; steady-state frames do not recreate them.
- normal non-capture frames perform no capture download, fence wait, mapping, normalization, or file I/O.
- renderer submission preserves caller-provided painter order.
- texture identity never participates in global draw-order sorting.
- culling uses the shared inclusive AABB rule.
- batching may only combine sprites already contiguous in the visible painter sequence unless equivalence is proven.
- `submittedSprites` and `drawCalls` remain independent metrics.
- texture validation semantics do not depend on camera visibility.
- capture frame selection uses simulation frame identity, never wall-clock timing.
- authored project/scene/UI state is text-first and deterministic.
- structured state beats pixel inference.
- semantic selectors beat coordinate targeting where identity exists.
- optimization complexity follows measurement.

## Handoff rule

Every PR that completes or materially changes an item in the owner-fixed execution order must update this file in the same PR.

A fresh coding agent following `AGENTS.md` should be able to read this file, select the first incomplete/unblocked item, and continue without previous chat history.
