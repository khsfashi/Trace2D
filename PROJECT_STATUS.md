# Trace2D Project Status

Last repository-state update: **2026-08-09**

This document is the operational handoff for the next contributor or coding agent. Live repository code, active PR state, and CI results win over stale prose.

## Current phase

**Public Alpha released — #40 deterministic texture assets, #42 text/basic UI, and #43 semantic UI automation are complete. #39 MCP transport is implemented by PR #58; after that PR merges green, #41 reproducible renderer workloads is the active next implementation task.**

`v0.1.0-alpha.1` was published on 2026-08-08. The repository is Public under the MIT License. Post-alpha work extends the proven agent-first loop rather than replacing it.

## Next execution order — owner-fixed

The repository owner fixed this sequence on **2026-08-08**. Future coding agents must follow it unless the owner explicitly changes it.

1. **#40 — deterministic texture asset cache/import slice** — complete via PR #45
2. **#42 — text rendering and basic UI primitives** — complete via PR #55
3. **#43 — semantic UI tree and agent interaction** — complete via PR #56
4. **#39 — MCP transport over the completed protocol-independent agent/UI facade** — implemented by PR #58; merge only after green CI
5. **#41 — reproducible renderer performance workloads** — **active next task after PR #58 merges**
6. **#47 — particle deterministic frame/keyed-random contracts**
7. **#48 — rich deterministic CPU particle reference simulation**
8. **#49 — text-authored particle effect assets + `ParticleEmitter2D`**
9. **#50 — complete Agent verification over CPU particle reference state**
10. **#51 — CPU particle cost analysis + explicit human backend choice + deterministic particle compiler**
11. **#52 — GPU runtime backend for explicitly GPU-selected effects**
12. **#53 — CPU/GPU conformance, workloads, safe budgets, build flow, and human/LLM guidance**
13. after #53, return to umbrella **#13** and split exactly one next breadth item: physics/Box2D, sprite animation, or safe hot reload

Particle umbrella: **#46**. Detailed particle contract: [`docs/PARTICLES.md`](docs/PARTICLES.md).

### Execution rule

- Work only on the first incomplete and unblocked item above.
- If that item has an active PR, finish/repair that PR before starting anything else.
- Merge only with green CI, then advance exactly one step.
- While PR #58 is open, finish/repair #58 and do not start #41.
- After PR #58 merges, #41 becomes the first incomplete task.
- Do **not** start #47 particles before #41 renderer workloads are complete.
- Within particles, complete exactly one of #47 -> #48 -> #49 -> #50 -> #51 -> #52 -> #53 at a time.
- MCP is transport, not the engine API.
- UI automation is semantic-first: stable identity/role/name plus structured state/actions. Coordinates are observable bounds, not the primary automation identity when semantic identity exists.
- Structured state beats pixel inference for gameplay, UI, and CPU-reference particle assertions.

**Active next implementation task after PR #58 merges: #41.**

## Completed #40 texture asset slice

The P6 asset foundation provides:

- deterministic project-relative texture identity with separator normalization,
- rejection of absolute paths and `..` traversal,
- PNG/JPEG/BMP/TGA decode to immutable RGBA8 CPU assets,
- successful-import caching with explicit invalidation/clear,
- stable structured diagnostics and cache metrics,
- no per-frame filesystem discovery/decoding,
- SDL-free `engine/assets` with no renderer/GPU ownership.

See [`docs/ASSETS.md`](docs/ASSETS.md).

## Completed #42 text/basic UI slice

The first UI slice established:

- SDL-free `engine/ui`,
- strict versioned `*.trace2d.toml` UI authoring,
- stable element IDs and deterministic authored order,
- `panel`, `label`, `button`, and `text_input`,
- deterministic integer pixel bounds,
- engine-owned focus and button activation state,
- deterministic dependency-free 5x7 ASCII-oriented text,
- caller-owned/reused RGBA8 CPU raster storage,
- headless and windowed preview paths over the same `UiDocument`,
- no renderer-owned authoritative UI state.

## Completed #43 semantic UI automation — PR #56

PR #56 extends the existing UI state rather than replacing it.

### Authored/state additions

- optional stable semantic `name`; if omitted it is resolved from the initial authored text at load time,
- `visible` state, default `true`,
- focused text-input replacement through engine-owned `UiDocument`,
- text mutation changes `text` but does not silently change semantic `name`,
- invisible elements reject focus/activation/text input and are skipped by CPU rasterization.

### Protocol-independent Agent surface

`AgentFacade` can bind a non-owning `UiDocument` and exposes:

```text
InspectUi
QueryUi
QueryOneUi
FocusUi
ActivateUi
InputUiText
AssertUi
```

UI snapshots expose stable ID, role, name, bounds, visible, enabled, focused, text, and activation count. `UiSelector` supports exact ID, role, and name criteria; multiple criteria are ANDed. `QueryUi` preserves authored order, and `QueryOneUi` reports stable no-match/ambiguity diagnostics.

Semantic actions resolve exactly one target before mutating the authoritative `UiDocument`. Text input requires prior focus. `AssertUi` checks visible/enabled/focused/text/activation-count state and returns structured mismatch context.

### Gameplay handoff

Button `activationCount` is the minimal deterministic edge signal. Gameplay can consume the count delta exactly once during deterministic update logic without a callback graph or heap event-object framework.

The committed headless game-interaction test proves semantic activation -> authoritative scene state change -> existing Agent scene verification with no screen-coordinate targeting or renderer initialization.

### Performance/scope contract

- element lookup and semantic queries remain deterministic O(N) authored-order scans,
- no speculative hash/index structure is added before measured need,
- `UiDocument` focus and activation state mutation add no heap allocation,
- explicit text replacement may resize its existing string and is not a per-frame hot path,
- Agent snapshots/results allocate only on explicit inspection/query/action requests,
- ordinary UI raster/simulation does not build JSON, MCP payloads, snapshots, or fingerprints.

See [`docs/UI.md`](docs/UI.md).

## Implemented #39 MCP transport — PR #58

PR #58 adds a deliberately thin adapter over the existing Agent/Testing vocabulary.

### Boundary

```text
Runtime / Scene / Input / UI
          |
          v
      AgentFacade
          |
          v
  GameplayScenario
          |
          v
      engine/mcp
          |
          v
   trace2d_mcp stdio host
```

`engine/mcp` owns protocol parsing/serialization only. JSON/MCP types do not enter core/runtime/scene/input/UI/agent/testing public contracts.

### Protocol surface

The primary protocol is MCP `2026-07-28`:

- `server/discover`,
- required modern per-request `_meta` version/capability fields,
- server identity in response `_meta`,
- deterministic cacheable `tools/list`,
- `tools/call`,
- newline-delimited stdio.

The adapter exposes fixed semantic tools for runtime/scene inspect/query, semantic UI inspect/query/focus/activate/text/assert, frame-indexed virtual input scheduling/inspection, explicit stepping, and existing gameplay float assertions.

The host also accepts the immediately preceding `initialize` request shape as a narrow stdio fallback probe. The modern contract remains `2026-07-28`.

### Determinism and failure behavior

- input continues to be scheduled by explicit simulation frame,
- stepping continues through `GameplayScenario::RunFrames`,
- runtime frame and seed remain engine-owned,
- scene/UI selectors continue to use existing semantic identity,
- gameplay and UI failures preserve existing structured diagnostics,
- unknown tools/malformed protocol requests remain JSON-RPC errors,
- one MCP step call is bounded to 100,000 frames without changing fixed-step semantics.

### Performance/scope contract

- MCP performs no JSON work unless a protocol message arrives,
- no MCP snapshot is retained between requests,
- tool-list order is stable and cacheable,
- nlohmann/json is private to the MCP adapter/tests,
- ordinary gameplay/UI/render paths acquire no MCP dependency or per-frame allocation,
- the stdio host initializes no renderer/GPU/window,
- HTTP/session/auth/editor frameworks remain out of scope.

### Headless verification

PR #58 protocol tests cover:

- modern discovery metadata and server identity,
- deterministic/cacheable tool listing,
- legacy initialize fallback response,
- MCP virtual input scheduling -> explicit stepping -> `#player` state change through an existing frame callback,
- semantic query and existing gameplay assertion after that state change,
- structured gameplay failure frame/seed/snapshot context,
- semantic UI focus/text/activation/assertion without renderer or coordinate targeting,
- structured UI mismatch context.

See [`docs/MCP.md`](docs/MCP.md).

## Why this order

Assets provide deterministic resource identity. Basic UI provides the smallest engine-owned UI state/rendering input. Semantic UI makes that same state directly inspectable and controllable. #39 then exposes that stable vocabulary through MCP as a transport adapter instead of forcing transport concerns into engine architecture.

#41 follows MCP to establish reproducible renderer workloads before the particle pipeline. Particles then use deterministic CPU reference behavior plus measured cost evidence before any explicit human-selected GPU backend.

## Particle target after #41

Particle implementation is governed by #46 and `docs/PARTICLES.md`.

The defining workflow remains:

```text
rich text effect
  -> deterministic CPU reference simulation
  -> full structured Agent verification
  -> deterministic structural CPU cost report
  -> optional machine-specific Release timing
  -> human chooses backend=cpu or backend=gpu
  -> GPU-selected effects compile to minimized GPU state
  -> CPU/GPU conformance + visual QA
```

Hard rules:

- CPU is the exact semantic reference,
- capacity is explicit and bounded,
- ordinary stepping creates no JSON/snapshot/fingerprint work unless requested,
- structural cost metrics and machine-specific timing are separated,
- an LLM may recommend a backend but never changes it automatically,
- backend choice is explicit reviewable human-controlled text,
- unsupported GPU effects fail clearly rather than silently falling back to CPU,
- normal GPU mode does not also run the full CPU reference simulation,
- GPU runtime state is minimized from compiler/static analysis,
- V1 does not claim universal cross-vendor bit-identical GPU floating point,
- visual particles never become gameplay authority.

## Public Alpha release record

- [x] P0-P5 technical milestones complete
- [x] Public Alpha vertical sample — PR #32
- [x] measured contiguous same-texture GPU instancing — PR #34
- [x] repository quality gates — PR #35
- [x] MIT license-required release gate — PR #36
- [x] release-ready documentation — PR #37
- [x] post-public documentation cleanup — PR #38
- [x] repository visibility Public
- [x] release/tag `v0.1.0-alpha.1`
- [x] root MIT `LICENSE`
- [x] Issue #14 closed completed

## Validation policy

Release-facing CI remains the baseline:

- `release-audit` using `./scripts/release_audit.ps1 -RequireLicense`
- `windows-msvc` configure/build/full CTest
- `clean-clone-quick-start` using the README-pinned vcpkg baseline

New machine-facing capabilities require deterministic automated tests when practical. GPU presentation itself is not a hosted-runner requirement; backend-independent UI/MCP state/query/actions/assertions/rasterization must remain headless-CI testable.

Wall-clock timing is environment-dependent evidence and must not become a deterministic correctness threshold.

## Architecture invariants

- `engine/core` has no SDL dependency.
- SDL-specific ownership/types stay behind platform/render boundaries.
- `engine/assets` and `engine/ui` are SDL-free.
- renderer GPU state is presentation state, never authoritative gameplay/UI state.
- runtime/scene/input/UI/agent/testing semantic state does not depend on MCP transport.
- MCP/JSON-RPC/CLI are adapters over protocol-independent engine/agent contracts.
- authored project/scene/UI/particle state is text-first and deterministic.
- semantic selectors beat coordinate targeting where identity exists.
- structured state beats pixel inference.
- persistent renderer resources are reused rather than recreated in steady state.
- capture frame selection uses simulation-frame identity, not wall-clock timing.
- texture identity never participates in global painter-order sorting.
- particle CPU state is the semantic oracle; GPU is an explicitly selected compiled backend.
- optimization complexity follows measurement.

## Handoff rule

Every PR that completes or materially changes an item in the owner-fixed execution order must update this file in the same PR.

A fresh coding agent following `AGENTS.md` should be able to read this file, inspect the active PR/CI, and continue without previous chat history.
