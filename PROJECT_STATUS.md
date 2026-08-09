# Trace2D Project Status

Last repository-state update: **2026-08-09**

This document is the operational handoff for the next contributor or coding agent. Live repository code, active PR state, and CI results win over stale prose.

## Current phase

**Public Alpha released — #40 deterministic texture assets, #42 text/basic UI, and #43 semantic UI automation are implemented. PR #56 is the active semantic-UI merge candidate; after it merges with green CI, #39 MCP transport is next.**

`v0.1.0-alpha.1` was published on 2026-08-08. The repository is Public under the MIT License. Post-alpha work extends the proven agent-first loop rather than replacing it.

## Next execution order — owner-fixed

The repository owner fixed this sequence on **2026-08-08**. Future coding agents must follow it unless the owner explicitly changes it.

1. **#40 — deterministic texture asset cache/import slice** — complete via PR #45
2. **#42 — text rendering and basic UI primitives** — complete via PR #55
3. **#43 — semantic UI tree and agent interaction** — implemented in PR #56; finish/merge this PR first
4. **#39 — MCP transport over the completed protocol-independent agent/UI facade**
5. **#41 — reproducible renderer performance workloads**
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
- Do **not** start #39 MCP before PR #56 / #43 is merged.
- Do **not** start #47 particles before #41 renderer workloads are complete.
- Within particles, complete exactly one of #47 -> #48 -> #49 -> #50 -> #51 -> #52 -> #53 at a time.
- MCP is transport, not the engine API.
- UI automation is semantic-first: stable identity/role/name plus structured state/actions. Coordinates are observable bounds, not the primary automation identity when semantic identity exists.
- Structured state beats pixel inference for gameplay, UI, and CPU-reference particle assertions.

**Active work: finish PR #56. Next implementation task after PR #56 merges: #39.**

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

## #43 semantic UI implementation — PR #56

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

UI snapshots expose:

- stable ID
- role
- name
- bounds
- visible
- enabled
- focused
- text
- activation count

`UiSelector` supports exact ID, role, and name criteria; multiple criteria are ANDed. `QueryUi` preserves authored order, and `QueryOneUi` reports stable no-match/ambiguity diagnostics.

V1 role mapping is:

```text
panel      -> panel
label      -> label
button     -> button
text_input -> textbox
```

Semantic actions resolve exactly one target before mutating the authoritative `UiDocument`. Text input requires prior focus. `AssertUi` checks visible/enabled/focused/text/activation-count state and returns structured mismatch context.

### Performance/scope contract

- element lookup and semantic queries remain deterministic O(N) authored-order scans,
- no speculative hash/index structure is added before measured need,
- focus and activation do not add heap allocation,
- explicit text replacement may resize its existing string and is not a per-frame hot path,
- Agent snapshots/results allocate only on explicit inspection/query requests,
- ordinary UI raster/simulation does not build JSON, MCP payloads, snapshots, or fingerprints,
- no DOM clone, browser abstraction, hierarchy/layout framework, coordinate-primary automation, or MCP implementation is introduced by #43.

### Headless verification

PR #56 adds tests that prove, without renderer initialization or coordinate targeting:

```text
query role=button name="Start Game"
  -> inspect semantic state
  -> activate twice
query role=textbox name="Player Name"
  -> focus
  -> input "Ada"
  -> assert focused/text state
```

Tests also cover authored-order multi-query determinism, invalid selectors, ambiguity, unbound UI, hidden/disabled controls, wrong control types, focus requirements, state-mismatch diagnostics, semantic TOML fields, and hidden-element raster behavior.

See [`docs/UI.md`](docs/UI.md).

## Why this order

Assets provide deterministic resource identity. Basic UI provides the smallest engine-owned UI state/rendering input. Semantic UI then makes that same state directly inspectable and controllable before MCP. #39 can therefore be a thin adapter over an already-complete protocol-independent vocabulary instead of forcing transport concerns into engine architecture.

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

New machine-facing capabilities require deterministic automated tests when practical. GPU presentation itself is not a hosted-runner requirement; backend-independent UI state/query/actions/assertions/rasterization must remain headless-CI testable.

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
